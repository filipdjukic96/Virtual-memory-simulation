#include "framemanager.h"
#include "vmallocator.h"
#include "clusterallocator.h"
#include "stdmutexlocker.h"
#include "part.h"

using namespace std;




FrameManager::FrameManager(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, Partition* partition) {
	this->partition = partition;
	clockHand = 0;

	clusterAllocator = new ClusterAllocator(partition);
	clusterDescriptors.resize(clusterAllocator->size(), nullptr);

	vmAllocator = new VMAllocator(processVMSpace, processVMSpaceSize);
	vmDescriptors.resize(vmAllocator->getSizeInFrames(), nullptr);
}

PhysicalAddress FrameManager::loadFrameInVM(PMT2Descriptor * pmt2Desc) {
	StdRecursiveMutexLocker locker(framesMutex);

	if (pmt2Desc == nullptr) {
		return nullptr;
	}
	else if (pmt2Desc->vmValid == 1){//pmt2Desc ima svoj alociran frame,vraca se adresa frame-a
	
		PhysicalAddress frameAdr = vmAllocator->getPhysicalAddressFromFrameNum(pmt2Desc->vmFrameNum);
		return frameAdr;
	}
	else {
		PhysicalAddress newFrameAdr = vmAllocator->allocateFrame();
		if (newFrameAdr == nullptr){
		
			PageNum flushedFrameNum = flushFrame();//bira se victimFrame i flush-uje,tj vraca na disk
			if (flushedFrameNum == UINT32_MAX) {
				return nullptr;
			}
			else {

			
				FrameDescriptor* victimFrameDescriptor = vmDescriptors[flushedFrameNum];
				clusterDescriptors[victimFrameDescriptor->getDiskFrameNum()] = victimFrameDescriptor;


				//oslobadja se izbaceni frame iz OM,i uklanja iz niza vmDescriptors
				vmAllocator->freeFrame(vmAllocator->getPhysicalAddressFromFrameNum(victimFrameDescriptor->getVMFrameNum()));
				vmDescriptors[flushedFrameNum] = nullptr;
				//resetuje mu se broj frame-a i valid bit
				victimFrameDescriptor->resetVMFrameNum();
				victimFrameDescriptor->resetVMValid();

				//alocira se novi frame,trebalo bi da to bude victimFrame(upravo izbaceni)
				newFrameAdr = vmAllocator->allocateFrame();
				if (newFrameAdr == nullptr) {
					return nullptr;
				}
			}
		}


		FrameDescriptor* newFrameDescriptor = nullptr;

		PageNum newFrameNum = vmAllocator->getFrameNumFromPhysicalAddress(newFrameAdr);
		
		if (pmt2Desc->diskValid) {
			//pmt2Desc ima svoj frame na disku,tj particiji(njegova stranica je na disku)
			//frameDesc tog frame-a se nalazi u nizu clusterDescriptors
			newFrameDescriptor = clusterDescriptors[pmt2Desc->diskFrameNum];
		}
		else {
			//u suprotnom,pokazivac na njegov frameDesc se cuva u polju diskFrameNum u pmt2Desc
			newFrameDescriptor = (FrameDescriptor*)pmt2Desc->diskFrameNum;
		}

		//ubacuje se frameDesc u vmDescriptors,setuje se valid bit i broj frame-a u OM
		vmDescriptors[newFrameNum] = newFrameDescriptor;
		newFrameDescriptor->setVMValid();
		newFrameDescriptor->setVMFrameNum(newFrameNum);

		//ako je ta stranica bila na disku,njen sadrzaj se mora dovuci u novoalocirani frame!!!
		if (newFrameDescriptor->getDiskValid()) {
			
			if (partition->readCluster(newFrameDescriptor->getDiskFrameNum(), (char*)newFrameAdr) == 0) {
				//nije uspjelo citanje sa particije,stranica neuspjesno procitana u frame,rollback
				vmAllocator->freeFrame(newFrameAdr);//oslobadja se alocirani frame
				vmDescriptors[newFrameNum] = nullptr;//izbacuje se frameDesc iz niza vmDescriptors
				newFrameDescriptor->remove(pmt2Desc);//izbacuje se ovaj pmt2Desc kao korisnik frame-a
				newFrameDescriptor->resetVMFrameNum();
				newFrameDescriptor->resetVMValid();

				if (pmt2Desc->diskValid) {
					clusterDescriptors[pmt2Desc->diskFrameNum] = newFrameDescriptor;
				}

				return nullptr;
			}
			else {
				return newFrameAdr;
			}
		}
		else {
			newFrameDescriptor->setDirty();
			return newFrameAdr;
		}
	}
}


//fja koja uklanja pmt2Desc kao korisnika frame-a(koji trenutno koristi)
void FrameManager::removeProcessFromFrameOwners(PMT2Descriptor * pmt2Desc) {
	StdRecursiveMutexLocker locker(framesMutex);

	if (pmt2Desc == nullptr) {
		return;
	}
	else {
		//pronalazi se frameDesc frame-a koji koristi ovaj pmt2Desc
		FrameDescriptor* frameDesc = frameDescFromPMT2Desc(pmt2Desc);
		if (frameDesc == nullptr) {
			return;
		}
		else {
			//uklanja se pmt2Desc kao korisnik frame-a
			frameDesc->remove(pmt2Desc);

			//resetuje se valid bit i broj frame-a
			pmt2Desc->vmValid = 0;
			pmt2Desc->vmFrameNum = 0;

			//resetuje se disk valid bit,polje diska nevalidno
			pmt2Desc->diskValid = 0;
			pmt2Desc->diskFrameNum = UINT32_MAX;

			removeProcessFromFrameOwners(frameDesc);
		}
	}
}

void FrameManager::removeProcessFromFrameOwners(FrameDescriptor* frameDesc) {
	StdRecursiveMutexLocker locker(framesMutex);
	if (frameDesc == nullptr){
		return;
	}
	//frame ima 0 korisnika,tj niko ga ne koristi
	else if (frameDesc->ownerCount() == 0){
		if (frameDesc->getVMValid()){
			//frame je u OM,oslobadja se iz nje,vraca se u vmAllocator
			PhysicalAddress frameAdr = vmAllocator->getPhysicalAddressFromFrameNum(frameDesc->getVMFrameNum());
			vmAllocator->freeFrame(frameAdr);
			//resetuje se valid bit i broj frame-a
			frameDesc->resetVMValid();
			frameDesc->resetVMFrameNum();
		}

		if (frameDesc->getDiskValid()){
			//frame je na disku(tj particiji),oslobadja se njegov cluster
			clusterAllocator->freeFrame(frameDesc->getDiskFrameNum());
			//resetuje se diskValid bit i broj cluster-a
			frameDesc->resetDiskValid();
			frameDesc->resetDiskFrameNum();
		}
	}
}


//fja za dovlacenje frameDesc odgovarajuceg pmt2Desc
FrameDescriptor * FrameManager::frameDescFromPMT2Desc(const PMT2Descriptor * pmt2Desc) {
	StdRecursiveMutexLocker locker(framesMutex);

	if (pmt2Desc == nullptr) {
		return nullptr;
	}
	else {
		FrameDescriptor* frameDesc = nullptr;
		if (pmt2Desc->vmValid == 1) {//frame je u OM
			return vmDescriptors[pmt2Desc->vmFrameNum];
		}
		else if (pmt2Desc->diskValid) {//frame je na particiji
			return clusterDescriptors[pmt2Desc->diskFrameNum];
		}
		else {
			return (FrameDescriptor*)pmt2Desc->diskFrameNum;
		}
	}
}


//dohvatanje victimPage
PageNum FrameManager::findFrameToFlush() {
	PageNum startCursor = clockHand;
	PageNum size = vmDescriptors.size();

	PageNum flushPos = UINT32_MAX;
	while (true) {
		if (vmDescriptors[clockHand]->getReferenceBit()) {
			vmDescriptors[clockHand]->resetReferenceBit();
			clockHand = (clockHand + 1) % size;
		}
		else {
			flushPos = clockHand;
			clockHand = (clockHand + 1) % size;
			break;
		}
	}

	return flushPos;
}


//fja koja pronalazi victimFrame(frame za izbacivanje) i vraca ga a njegov cluster(postojeci ili alocira novi)
PageNum FrameManager::flushFrame()
{
	PageNum victimFrameNum = findFrameToFlush();//victimFrame po clock alg
	//uzima se frameDesc victimFrame-a
	FrameDescriptor* victimFrameDescriptor = vmDescriptors[victimFrameNum];

	//adresa victimFrame-a
	PhysicalAddress victimFrameAdr = vmAllocator->getPhysicalAddressFromFrameNum(victimFrameNum);
	//ako victimFrame nema svoj cluster na disku,alocira se novi i stranica upisuje na disk
	if (victimFrameDescriptor->getDiskValid() == false) {
		//alocira se novi cluster za frame
		PageNum diskFrameNum = clusterAllocator->allocateFrame();
		if (diskFrameNum != UINT32_MAX) {
			//upis stranice na cluster
			bool result = partition->writeCluster(diskFrameNum, (const char*)victimFrameAdr) != 0;
			if (result == true) {
				
				//uspjela alokacija clustera i upis na njega,postavlja se diskValid=1
				victimFrameDescriptor->setDiskValid();
				//postavlja se broj diska
				victimFrameDescriptor->setDiskFrameNum(diskFrameNum);
			}
			else {
				clusterAllocator->freeFrame(diskFrameNum);
				return UINT32_MAX;
			}
		}
	}
	//frame ima svoj cluster na disku
	//ukoliko je frame zaprljan,tj. dirty,upisujemo ga na cluster i resetujemo dirty bit
	else if (victimFrameDescriptor->getDirty()) {
		if (partition->writeCluster(victimFrameDescriptor->getDiskFrameNum(), (const char*)victimFrameAdr) != 0) {
			victimFrameDescriptor->resetDirty();
		}
	}

	return victimFrameNum;
}
